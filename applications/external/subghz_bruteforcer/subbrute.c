#include "subbrute_i.h"
#include "scenes/subbrute_scene.h"

#define TAG "SubBruteApp"

static bool subbrute_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    SubBruteState* instance = context;
    return scene_manager_handle_custom_event(instance->scene_manager, event);
}

static bool subbrute_back_event_callback(void* context) {
    furi_assert(context);
    SubBruteState* instance = context;
    return scene_manager_handle_back_event(instance->scene_manager);
}

static void subbrute_tick_event_callback(void* context) {
    furi_assert(context);
    SubBruteState* instance = context;
    scene_manager_handle_tick_event(instance->scene_manager);
}

SubBruteState* subbrute_alloc() {
    SubBruteState* instance = malloc(sizeof(SubBruteState));

    memset(instance->text_store, 0, sizeof(instance->text_store));
    instance->file_path = furi_string_alloc();

    instance->scene_manager = scene_manager_alloc(&subbrute_scene_handlers, instance);
    instance->view_dispatcher = view_dispatcher_alloc();

    instance->gui = furi_record_open(RECORD_GUI);

    view_dispatcher_set_event_callback_context(instance->view_dispatcher, instance);
    view_dispatcher_set_custom_event_callback(
        instance->view_dispatcher, subbrute_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        instance->view_dispatcher, subbrute_back_event_callback);
    view_dispatcher_set_tick_event_callback(
        instance->view_dispatcher, subbrute_tick_event_callback, 100);

    //Dialog
    instance->dialogs = furi_record_open(RECORD_DIALOGS);

    // Notifications
    instance->notifications = furi_record_open(RECORD_NOTIFICATION);

    subghz_devices_init();

    // init radio device
    instance->radio_device =
        subbrute_radio_device_loader_set(NULL, SubGhzRadioDeviceTypeExternalCC1101);

    subghz_devices_reset(instance->radio_device);
    subghz_devices_idle(instance->radio_device);

    // Devices
    instance->device = subbrute_device_alloc(instance->radio_device);

    // SubBruteWorker
    instance->worker = subbrute_worker_alloc(instance->radio_device);

    // TextInput
    instance->text_input = text_input_alloc();
    view_dispatcher_add_view(
        instance->view_dispatcher,
        SubBruteViewTextInput,
        text_input_get_view(instance->text_input));

    // VarList
    instance->var_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        instance->view_dispatcher,
        SubBruteViewVarList,
        variable_item_list_get_view(instance->var_list));

    // Popup
    instance->popup = popup_alloc();
    view_dispatcher_add_view(
        instance->view_dispatcher, SubBruteViewPopup, popup_get_view(instance->popup));

    // SubBruteMainView
    instance->view_main = subbrute_main_view_alloc();
    view_dispatcher_add_view(
        instance->view_dispatcher,
        SubBruteViewMain,
        subbrute_main_view_get_view(instance->view_main));

    // SubBruteAttackView
    instance->view_attack = subbrute_attack_view_alloc();
    view_dispatcher_add_view(
        instance->view_dispatcher,
        SubBruteViewAttack,
        subbrute_attack_view_get_view(instance->view_attack));

    instance->settings = subbrute_settings_alloc();
    subbrute_settings_load(instance->settings);

    // Uncomment to enable Debug pin output on PIN 17(1W)
    // subghz_devices_set_async_mirror_pin(instance->radio_device, &gpio_ibutton);

    return instance;
}

void subbrute_free(SubBruteState* instance) {
    furi_assert(instance);

    // Uncomment to enable Debug pin output on PIN 17(1W)
    // subghz_devices_set_async_mirror_pin(instance->radio_device, NULL);

    // SubBruteWorker (stops and joins thread, ends radio device)
    subbrute_worker_free(instance->worker);

    // SubBruteDevice
    subbrute_device_free(instance->device);
    subghz_devices_deinit();

    subbrute_settings_free(instance->settings);

    // Notifications
    notification_message(instance->notifications, &sequence_blink_stop);
    furi_record_close(RECORD_NOTIFICATION);
    instance->notifications = NULL;

    // View Main
    view_dispatcher_remove_view(instance->view_dispatcher, SubBruteViewMain);
    subbrute_main_view_free(instance->view_main);

    // View Attack
    view_dispatcher_remove_view(instance->view_dispatcher, SubBruteViewAttack);
    subbrute_attack_view_free(instance->view_attack);

    // TextInput
    view_dispatcher_remove_view(instance->view_dispatcher, SubBruteViewTextInput);
    text_input_free(instance->text_input);

    // VarList
    view_dispatcher_remove_view(instance->view_dispatcher, SubBruteViewVarList);
    variable_item_list_free(instance->var_list);

    // Popup
    view_dispatcher_remove_view(instance->view_dispatcher, SubBruteViewPopup);
    popup_free(instance->popup);

    // Dialog
    furi_record_close(RECORD_DIALOGS);
    instance->dialogs = NULL;

    // Scene manager
    scene_manager_free(instance->scene_manager);

    // View Dispatcher
    view_dispatcher_free(instance->view_dispatcher);

    // GUI
    furi_record_close(RECORD_GUI);
    instance->gui = NULL;

    furi_string_free(instance->file_path);

    // The rest
    free(instance);
}

void subbrute_text_input_callback(void* context) {
    furi_assert(context);
    SubBruteState* instance = context;
    view_dispatcher_send_custom_event(
        instance->view_dispatcher, SubBruteCustomEventTypeTextEditDone);
}

void subbrute_popup_closed_callback(void* context) {
    furi_assert(context);
    SubBruteState* instance = context;
    view_dispatcher_send_custom_event(
        instance->view_dispatcher, SubBruteCustomEventTypePopupClosed);
}

void subbrute_apply_extra_settings(SubBruteState* instance) {
    furi_assert(instance);
    const SubBruteAttacks attack = instance->device->attack;
    SubBruteSettings* settings = instance->settings;

    subbrute_worker_set_timeout(instance->worker, settings->time_delay[attack]);

    // TE and open code of a loaded file belong to that file, not to the protocol slot
    if(attack == SubBruteAttackLoadFile) {
        return;
    }

    if(settings->te[attack] != 0 && subbrute_worker_get_te(instance->worker) != 0) {
        subbrute_worker_set_te(instance->worker, settings->te[attack]);
    }

    if(subbrute_worker_get_is_pt2262(instance->worker)) {
        subbrute_worker_set_opencode(instance->worker, settings->opencode[attack]);
        instance->device->opencode = settings->opencode[attack];
    }
}

void subbrute_store_extra_settings(SubBruteState* instance) {
    furi_assert(instance);
    const SubBruteAttacks attack = instance->device->attack;
    SubBruteSettings* settings = instance->settings;
    const uint8_t repeats = subbrute_worker_get_repeats(instance->worker);

    settings->time_delay[attack] = subbrute_worker_get_timeout(instance->worker);
    settings->repeat_values[attack] = repeats;
    instance->device->extra_repeats = repeats;

    if(attack != SubBruteAttackLoadFile) {
        const uint32_t te = subbrute_worker_get_te(instance->worker);
        // Keep 0 when TE equals protocol default, so default changes still apply
        settings->te[attack] = te == subbrute_protocol(attack)->te ? 0 : te;

        if(subbrute_worker_get_is_pt2262(instance->worker)) {
            settings->opencode[attack] = subbrute_worker_get_opencode(instance->worker);
        }
    }

    subbrute_settings_save(settings);
}

// ENTRYPOINT
int32_t subbrute_app(void* p) {
    UNUSED(p);
    furi_hal_power_suppress_charge_enter();

    SubBruteState* instance = subbrute_alloc();
    view_dispatcher_attach_to_gui(
        instance->view_dispatcher, instance->gui, ViewDispatcherTypeFullscreen);
    scene_manager_next_scene(instance->scene_manager, SubBruteSceneStart);

    notification_message(instance->notifications, &sequence_display_backlight_on);
    view_dispatcher_run(instance->view_dispatcher);

    subbrute_free(instance);

    furi_hal_power_suppress_charge_exit();

    return 0;
}
