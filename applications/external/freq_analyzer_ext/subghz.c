/* Abandon hope, all ye who enter here. */

#include <furi/core/log.h>
#include <subghz/types.h>
#include <lib/toolbox/path.h>
#include <float_tools.h>
#include "subghz_i.h"

#define TAG "SubGhzApp"

bool subghz_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    SubGhz* subghz = context;
    return scene_manager_handle_custom_event(subghz->scene_manager, event);
}

bool subghz_back_event_callback(void* context) {
    furi_assert(context);
    SubGhz* subghz = context;
    return scene_manager_handle_back_event(subghz->scene_manager);
}

void subghz_tick_event_callback(void* context) {
    furi_assert(context);
    SubGhz* subghz = context;
    scene_manager_handle_tick_event(subghz->scene_manager);
}

SubGhz* subghz_alloc() {
    SubGhz* subghz = malloc(sizeof(SubGhz));

    // GUI
    subghz->gui = furi_record_open(RECORD_GUI);

    // View Dispatcher
    subghz->view_dispatcher = view_dispatcher_alloc();

    subghz->scene_manager = scene_manager_alloc(&subghz_scene_handlers, subghz);
    view_dispatcher_set_event_callback_context(subghz->view_dispatcher, subghz);
    view_dispatcher_set_custom_event_callback(
        subghz->view_dispatcher, subghz_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        subghz->view_dispatcher, subghz_back_event_callback);
    view_dispatcher_set_tick_event_callback(
        subghz->view_dispatcher, subghz_tick_event_callback, 100);

    // Open Notification record
    subghz->notifications = furi_record_open(RECORD_NOTIFICATION);

    subghz->txrx = subghz_txrx_alloc();

    // Frequency Analyzer
    // View knows too much
    subghz->subghz_frequency_analyzer = subghz_frequency_analyzer_alloc(subghz->txrx);
    view_dispatcher_add_view(
        subghz->view_dispatcher,
        SubGhzViewIdFrequencyAnalyzer,
        subghz_frequency_analyzer_get_view(subghz->subghz_frequency_analyzer));

    // Load last used values for Read, Read RAW, etc. or default
    subghz->last_settings = subghz_last_settings_alloc();

    subghz_last_settings_load(subghz->last_settings);

    subghz->filter = SubGhzProtocolFlag_Decodable;
    subghz->ignore_filter = 0x0;

    subghz_txrx_receiver_set_filter(subghz->txrx, subghz->filter);

    return subghz;
}

void subghz_free(SubGhz* subghz) {
    furi_assert(subghz);

    subghz_txrx_speaker_off(subghz->txrx);
    subghz_txrx_stop(subghz->txrx);
    subghz_txrx_sleep(subghz->txrx);

    // Frequency Analyzer
    view_dispatcher_remove_view(subghz->view_dispatcher, SubGhzViewIdFrequencyAnalyzer);
    subghz_frequency_analyzer_free(subghz->subghz_frequency_analyzer);

    // Scene manager
    scene_manager_free(subghz->scene_manager);

    // View Dispatcher
    view_dispatcher_free(subghz->view_dispatcher);

    // GUI
    furi_record_close(RECORD_GUI);
    subghz->gui = NULL;

    subghz_last_settings_free(subghz->last_settings);

    //TxRx
    subghz_txrx_free(subghz->txrx);

    // Notifications
    furi_record_close(RECORD_NOTIFICATION);
    subghz->notifications = NULL;

    // The rest
    free(subghz);
}

int32_t freq_anal_app(void* p) {
    UNUSED(p);
    SubGhz* subghz = subghz_alloc();

    view_dispatcher_attach_to_gui(
        subghz->view_dispatcher, subghz->gui, ViewDispatcherTypeFullscreen);
    scene_manager_next_scene(subghz->scene_manager, SubGhzSceneFrequencyAnalyzer);

    furi_hal_power_suppress_charge_enter();

    view_dispatcher_run(subghz->view_dispatcher);

    furi_hal_power_suppress_charge_exit();

    subghz_free(subghz);

    return 0;
}
