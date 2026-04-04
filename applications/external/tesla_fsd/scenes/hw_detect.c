#include "../tesla_fsd_app.h"
#include "../scenes_config/app_scene_functions.h"

#define DETECT_TIMEOUT_MS 8000
#define DETECT_POLL_MS    5

static int32_t hw_detect_worker(void* context) {
    TeslaFSDApp* app = context;
    MCP2515* mcp = app->mcp_can;
    CANFRAME frame;

    mcp->mode = MCP_NORMAL;
    mcp->bitRate = MCP_500KBPS;
    mcp->clck = MCP_16MHZ;

    if(mcp2515_init(mcp) != ERROR_OK) {
        view_dispatcher_send_custom_event(app->view_dispatcher, TeslaFSDEventNoDevice);
        return 0;
    }

    // Filter only CAN ID 0x398 (GTW_carConfig)
    init_mask(mcp, 0, 0x7FF);
    init_filter(mcp, 0, CAN_ID_GTW_CAR_CONFIG);
    init_filter(mcp, 1, CAN_ID_GTW_CAR_CONFIG);
    init_mask(mcp, 1, 0x7FF);
    init_filter(mcp, 2, CAN_ID_GTW_CAR_CONFIG);
    init_filter(mcp, 3, CAN_ID_GTW_CAR_CONFIG);
    init_filter(mcp, 4, CAN_ID_GTW_CAR_CONFIG);
    init_filter(mcp, 5, CAN_ID_GTW_CAR_CONFIG);

    uint32_t start = furi_get_tick();

    while((furi_get_tick() - start) < furi_ms_to_ticks(DETECT_TIMEOUT_MS)) {
        uint32_t flags = furi_thread_flags_get();
        if(flags & WorkerFlagStop) {
            deinit_mcp2515(mcp);
            return 0;
        }

        if(check_receive(mcp) == ERROR_OK) {
            if(read_can_message(mcp, &frame) == ERROR_OK) {
                TeslaHWVersion hw = fsd_detect_hw_version(&frame);
                if(hw != TeslaHW_Unknown) {
                    furi_mutex_acquire(app->mutex, FuriWaitForever);
                    app->hw_version = hw;
                    fsd_state_init(&app->fsd_state, hw);
                    furi_mutex_release(app->mutex);

                    deinit_mcp2515(mcp);
                    view_dispatcher_send_custom_event(app->view_dispatcher, TeslaFSDEventHWDetected);
                    return 0;
                }
            }
        }
        furi_delay_ms(DETECT_POLL_MS);
    }

    deinit_mcp2515(mcp);
    view_dispatcher_send_custom_event(app->view_dispatcher, TeslaFSDEventHWNotFound);
    return 0;
}

void tesla_fsd_scene_hw_detect_on_enter(void* context) {
    TeslaFSDApp* app = context;

    widget_reset(app->widget);
    widget_add_string_multiline_element(
        app->widget, 64, 20, AlignCenter, AlignCenter, FontPrimary,
        "Detecting HW...");
    widget_add_string_multiline_element(
        app->widget, 64, 40, AlignCenter, AlignCenter, FontSecondary,
        "Listening for\nGTW_carConfig (0x398)");

    view_dispatcher_switch_to_view(app->view_dispatcher, TeslaFSDViewWidget);

    app->worker_thread = furi_thread_alloc_ex("TeslaHWDetect", 2048, hw_detect_worker, app);
    furi_thread_start(app->worker_thread);
}

bool tesla_fsd_scene_hw_detect_on_event(void* context, SceneManagerEvent event) {
    TeslaFSDApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case TeslaFSDEventHWDetected:
            scene_manager_next_scene(app->scene_manager, tesla_fsd_scene_fsd_running);
            consumed = true;
            break;
        case TeslaFSDEventHWNotFound:
            widget_reset(app->widget);
            widget_add_string_multiline_element(
                app->widget, 64, 20, AlignCenter, AlignCenter, FontPrimary,
                "HW Not Detected");
            widget_add_string_multiline_element(
                app->widget, 64, 44, AlignCenter, AlignCenter, FontSecondary,
                "Go back and\nselect manually");
            consumed = true;
            break;
        case TeslaFSDEventNoDevice:
            widget_reset(app->widget);
            widget_add_string_multiline_element(
                app->widget, 64, 28, AlignCenter, AlignCenter, FontPrimary,
                "CAN Module\nNot Found");
            consumed = true;
            break;
        }
    }
    return consumed;
}

void tesla_fsd_scene_hw_detect_on_exit(void* context) {
    TeslaFSDApp* app = context;

    if(app->worker_thread) {
        furi_thread_flags_set(furi_thread_get_id(app->worker_thread), WorkerFlagStop);
        furi_thread_join(app->worker_thread);
        furi_thread_free(app->worker_thread);
        app->worker_thread = NULL;
    }
    widget_reset(app->widget);
}
